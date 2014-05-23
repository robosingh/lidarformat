/***********************************************************************

This file is part of the LidarFormat project source files.

LidarFormat is an open source library for efficiently handling 3D point 
clouds with a variable number of attributes at runtime. 


Homepage: 

    http://code.google.com/p/lidarformat

Copyright:

    Institut Geographique National & CEMAGREF (2009)

Author: 

    Adrien Chauve

Contributors:

    Nicolas David, Olivier Tournaire



    LidarFormat is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    LidarFormat is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with LidarFormat.  If not, see <http://www.gnu.org/licenses/>.

***********************************************************************/



#include <stdexcept>
#include <fstream>
#include <algorithm>

#include <boost/bind.hpp>
#include <boost/bind/placeholders.hpp>

#include <boost/noncopyable.hpp>

#include "LidarFormat/LidarDataFormatTypes.h"
#include "LidarFormat/tools/AttributeBounds.h"
#include "apply.h"

#include "LidarDataContainer.h"




namespace Lidar
{

void LidarDataContainer::clear()
{
    lidarData_.clear();
}


std::ostream &LidarDataContainer::printHeader(std::ostream &os) const
{
    for (AttributeMapType::iterator it = attributeMap_->begin(); it != attributeMap_->end(); ++it)
    {
        os << it->first << "\t";
    }
    os << std::endl;
    return os;
}


void LidarDataContainer::getAttributeList(std::vector<std::string> &liste) const
{
    liste.clear();
    std::transform(attributeMap_->begin(), attributeMap_->end(), std::back_inserter(liste),
                   boost::bind( &AttributeMapType::value_type::first, _1 )
                   );
}

bool LidarDataContainer::getAttributeBounds(const std::string &attributeName, double & min, double & max) const
{
    AttributeMapType::iterator it = attributeMap_->find(attributeName);
    assert(it != attributeMap_->end());
    AttributesInfo & info = it->second;
    if(info.dirty) return false;
    min = info.min; max = info.max;
    return true;
}

void LidarDataContainer::getAttributeCleanBounds(const std::string &attributeName, double & min, double & max, bool force_recompute)
{
    AttributeMapType::iterator it = attributeMap_->find(attributeName);
    assert(it != attributeMap_->end());
    AttributesInfo & info = it->second;
    if(force_recompute || info.dirty)
    {
        FonctorMultiAbstractBound fmab;
        fmab.AddAttribute(it->first, info.decalage, info.type);
        fmab = std::for_each (begin(), end(), fmab);
        fmab.mvp_attrib[0]->Get(info.min, info.max);
        info.dirty = false;
    }
    min = info.min; max = info.max;
}

void LidarDataContainer::recomputeBounds(bool force_recompute)
{
    FonctorMultiAbstractBound fmab;
    for(AttributeMapType::iterator it = attributeMap_->begin(); it != attributeMap_->end(); it++)
        if(force_recompute || it->second.dirty)
        {
            fmab.AddAttribute(it->first, it->second.decalage, it->second.type);
        }
    fmab = std::for_each (begin(), end(), fmab);
    int i=0;
    for(AttributeMapType::iterator it = attributeMap_->begin(); it != attributeMap_->end(); it++)
        if(force_recompute || it->second.dirty)
        {
            fmab.mvp_attrib[i++]->Get(it->second.min, it->second.max);
            it->second.dirty = false;
        }
}

LidarDataContainer::LidarDataContainer():
    attributeMap_(new AttributeMapType)
{

}

LidarDataContainer::LidarDataContainer(const LidarDataContainer& rhs):
    attributeMap_(new AttributeMapType)
{
    copy(rhs);
}

LidarDataContainer& LidarDataContainer::operator=(const LidarDataContainer& rhs)
{
    if(this!=&rhs)
        copy(rhs);

    return *this;
}

void LidarDataContainer::copy(const LidarDataContainer& rhs)
{
    *attributeMap_ = *rhs.attributeMap_;

    lidarData_ = rhs.lidarData_;
    pointSize_ = rhs.pointSize_;
}


void LidarDataContainer::append(const LidarDataContainer& rhs)
{
    //	assert(*rhs.attributeMap_ == *attributeMap_);

    lidarData_.insert(lidarData_.end(), rhs.lidarData_.begin(), rhs.lidarData_.end());
}

//struct FunctorAddAttributeParameters
//{
//	explicit FunctorAddAttributeParameters(const std::string& name, AttributeMapType& attributeMap):
//		name_(name), attributeMap_(attributeMap){}
//	const std::string& name_;
//	AttributeMapType& attributeMap_;
//};
//
//template<EnumLidarDataType TAttributeType>
//struct FunctorAddAttribute
//{
//	typedef typename Lidar::LidarEnumTypeTraits<TAttributeType>::type AttributeType;
//
//	unsigned int operator()(const FunctorAddAttributeParameters& p)
//	{
//		AttributesInfo infos;
//		infos.type = LidarTypeTraits<AttributeType>::enum_type;
//
//		if(p.attributeMap_.empty())
//			infos.decalage = 0;
//		else
//			infos.decalage = p.attributeMap_.back().second.decalage + sizeof(AttributeType);
//
//		p.attributeMap_.push_back(AttributeMapType::value_type(p.name_, infos));
//
//		return sizeof(AttributeType);
//	}
//
//};


template<EnumLidarDataType T>
struct PointSizeFunctor
{
    unsigned int operator()()
    {
        return sizeof( typename LidarEnumTypeTraits<T>::type );
    }
};


void LidarDataContainer::updateAttributeContent(const unsigned int oldPointSize)
{
    lidarData_.resize(lidarData_.size()/oldPointSize*pointSize_);

    ///MAJ des données ! décalage des attributs + initialisation du nouvel attribut
    const LidarDataContainerType::const_iterator itBegin = lidarData_.begin();
    LidarDataContainerType::const_iterator itOldEndElement = itBegin + (size()-1)*oldPointSize;

    LidarDataContainerType::iterator itNewEndElement = lidarData_.begin() + (size()-1)*pointSize_;

    while(itOldEndElement > itBegin)
    {
        std::copy(itOldEndElement, itOldEndElement + oldPointSize, itNewEndElement);
        itOldEndElement -= oldPointSize;
        itNewEndElement -= pointSize_;
    }
}

bool LidarDataContainer::addAttributeHelper(const std::string& attributeName, const EnumLidarDataType type,
                                            bool dirty, double min, double max)
{
    //si l'attribut existe déjà, on sort et retourne false
    if(attributeMap_->find(attributeName)!=attributeMap_->end())
        return false;


    //calcul du nouveau décalage
    unsigned int sizeLastAttribute=0;

    if(!attributeMap_->empty())
        sizeLastAttribute = apply<PointSizeFunctor, unsigned int>(attributeMap_->back().second.type);

    //ajout de l'attribut dans les maps
    AttributesInfo infos;
    infos.type = type;
    if(!attributeMap_->empty())
    {
        infos.decalage = sizeLastAttribute + attributeMap_->back().second.decalage;
    }
    else
        infos.decalage = sizeLastAttribute;
    infos.dirty = dirty;
    infos.min = min;
    infos.max = max;

    attributeMap_->push_back(AttributeMapType::value_type(attributeName, infos));

    //Mise à jour de la pointSize :
    unsigned int sizeLastInsertedAttribute;
    sizeLastInsertedAttribute = apply<PointSizeFunctor, unsigned int>(type);
    pointSize_ = infos.decalage + sizeLastInsertedAttribute;

    return true;
}


bool LidarDataContainer::addAttribute(const std::string& attributeName, const EnumLidarDataType type,
                                      bool dirty, double min, double max)
{
    const unsigned int oldPointSize = pointSize_;

    bool attributeAdded = addAttributeHelper(attributeName, type, dirty, min, max);

    if(!attributeAdded)
        return false;

    if(!empty())
        updateAttributeContent(oldPointSize);

    return true;
}


void LidarDataContainer::addAttributeList(const std::vector<std::pair<std::string, EnumLidarDataType> > attributes)
{
    const unsigned int oldPointSize = pointSize_;


    std::vector<std::pair<std::string, EnumLidarDataType> >::const_iterator itb = attributes.begin();
    const std::vector<std::pair<std::string, EnumLidarDataType> >::const_iterator ite = attributes.end();


    for( ; itb != ite; ++itb)
        addAttributeHelper(itb->first, itb->second);

    if(!empty())
        updateAttributeContent(oldPointSize);
}

struct predicate_true
{
    template<typename T>
    bool operator()(T)
    {
        return true;
    }
};


bool LidarDataContainer::delAttribute(const std::string& attributeName)
{
    //si l'attribut n'existe pas, on sort
    if (!checkAttributeIsPresent(attributeName))
        return false;

    //décalage et taille de l'attribut à supprimer
    const unsigned int strideAttribute = getDecalage(attributeName);
    const unsigned int sizeAttribute = apply<PointSizeFunctor, unsigned int>(getAttributeType(attributeName));

    //Mise à jour de la pointSize :
    const unsigned int oldPointSize = pointSize_;
    const unsigned int nbPoints = size();
    pointSize_ -= sizeAttribute;


    ///MAJ des données ! décalage des attributs
    LidarDataContainerType::iterator itOldAttributePosition = lidarData_.begin() + strideAttribute;
    LidarDataContainerType::iterator itNewAttributePosition = lidarData_.begin() + strideAttribute;
    const LidarDataContainerType::iterator ite = lidarData_.end() - oldPointSize;

    for(; itOldAttributePosition < ite; itOldAttributePosition += oldPointSize, itNewAttributePosition += pointSize_)
        std::copy(itOldAttributePosition + sizeAttribute, itOldAttributePosition + oldPointSize, itNewAttributePosition);


    lidarData_.erase(lidarData_.begin() + nbPoints*pointSize_, lidarData_.end());


    //suppression de l'attribut dans les maps et mise à jour des décalages qui suivent l'attribut supprimé
    AttributeMapType::iterator itSuccessor = attributeMap_->erase(attributeMap_->find(attributeName));

    for(; itSuccessor != attributeMap_->end(); ++itSuccessor)
        itSuccessor->second.decalage -= sizeAttribute;

    return true;
}

void LidarDataContainer::delAttributeList(const std::vector<std::string>& attributeNames)
{
    for(std::vector<std::string>::const_iterator it = attributeNames.begin(); it != attributeNames.end(); ++it)
        delAttribute(*it);
}


} //namespace Lidar
